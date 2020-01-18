#include "server_upload.h"

#include <net/net_ip.h>
#include <net/socket.h>
#include <pb_encode.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(srv);


#include "dogcollar.pb.h"

// TODO: Externalize config
const char* hostname = "0.tcp.ngrok.io";
const char* port = "18785";


static bool write_roaring_bitmap(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg)
{
    size_t bytes_needed_to_serialize = roaring_bitmap_portable_size_in_bytes(*arg);
    char* serialized_activity_bitmap = malloc(bytes_needed_to_serialize);
    if (!serialized_activity_bitmap) {
        LOG_WRN("Unable to allocate memory for serialized activity history");
        return false;
    }
    size_t bytes_serialized = roaring_bitmap_portable_serialize(*arg, serialized_activity_bitmap);
    
    if (bytes_needed_to_serialize != bytes_serialized) {
        LOG_WRN("Error serializing activity history: serialized data should have used %d bytes, but used %d", bytes_needed_to_serialize, bytes_serialized);
        return false;
    }

    bool result = pb_encode_tag_for_field(stream, field) && pb_encode_string(stream, (uint8_t*)serialized_activity_bitmap, bytes_serialized);

    free(serialized_activity_bitmap);

    return result;
}


static int setup_socket(sa_family_t family,
			int *sock, struct addrinfo *addr)
{
	const char *family_str = family == AF_INET ? "IPv4" : "IPv6";
	int ret = 0;

	*sock = socket(family, SOCK_STREAM, IPPROTO_TCP);

	if (*sock < 0) {
		LOG_ERR("Failed to create %s socket (%d)", family_str,
			-errno);
	}

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	return ret;
}

static int connect_socket(sa_family_t family,
			  int *sock, struct addrinfo *addr)
{
	int ret;

	ret = setup_socket(family, sock, addr);
	if (ret < 0 || *sock < 0) {
		return -1;
	}

	ret = connect(*sock, addr->ai_addr, addr->ai_addrlen);
	if (ret < 0) {
		LOG_ERR("Cannot connect to %s remote (%d)",
			family == AF_INET ? "IPv4" : "IPv6",
			-errno);
		ret = -errno;
	}

	return ret;
}

static bool write_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    int fd = (intptr_t)stream->state;

    int remaining = count;
    const uint8_t* current_buf = buf;
    while(remaining > 0) {
        int bytes_written = send(fd, current_buf, remaining, 0);
        if (bytes_written <= 0) {
            LOG_ERR("Error writing protobuf message: %d", bytes_written);
            return false;
        }
        remaining -= bytes_written;
        current_buf += bytes_written;
    }

    return true;
}

pb_ostream_t pb_ostream_from_socket(int fd)
{
    pb_ostream_t stream = {&write_callback, (void*)(intptr_t)fd, SIZE_MAX, 0};
    return stream;
}

int report_activity_history(time_t start_timestamp, roaring_bitmap_t* activity_bitmap) {
    bool bitmap_optimized = roaring_bitmap_run_optimize(activity_bitmap);
    if (bitmap_optimized) {
        LOG_DBG("Optimized activity bitmap");
    }

    LOG_INF("Looking up IP address for %s", hostname);
    static struct addrinfo hints;
	struct addrinfo *res;
    hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
    

    int ret = getaddrinfo(hostname, port, &hints, &res);
    if (ret) {
        LOG_ERR("Unable to lookup hostname: %d", ret);
        return -EIO;
    }

    int sock4 = -1;
    LOG_INF("Opening socket");
    connect_socket(AF_INET,
				     &sock4, res);
    if (sock4 < 0) {
        LOG_ERR("Unable to connect to server");
        return -EIO;
    }

    Activity activity = Activity_init_zero;
    activity.timestamp = start_timestamp;
    activity.serialized_roaring_bitmap.funcs.encode = &write_roaring_bitmap;
    activity.serialized_roaring_bitmap.arg = activity_bitmap;


    pb_ostream_t output = pb_ostream_from_socket(sock4);
    if (!pb_encode_delimited(&output, Activity_fields, &activity)) {
        LOG_ERR("Unable to write message to server");
    }

    close(sock4);

    LOG_INF("Activity history uploaded");

    return 0;
}