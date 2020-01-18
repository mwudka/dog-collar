package main

import (
	"server/dogcollar"
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"github.com/RoaringBitmap/roaring"
	"github.com/gogo/protobuf/proto"
	"io"
	"net"
	"os"
)

func main() {
	fmt.Printf("Started ProtoBuf Server")
	c := make(chan *dogcollar.Activity)
	go func(){
		for{
			message := <-c

			fmt.Printf("Got message %+v\n", message)

			bitmap := roaring.New()
			_, err := bitmap.ReadFrom(bytes.NewReader(message.SerializedRoaringBitmap))
			if err != nil {
				fmt.Printf("Error reading bitmap: %+v\n", err)
			} else {
				iterator := bitmap.Iterator()
				for iterator.HasNext() {
					u := *message.Timestamp + uint64(iterator.Next())
					fmt.Printf("Activity reported at %d\n", u)
				}

			}

		}
	}()
	//Listen to the TCP port
	listener, err := net.Listen("tcp", "127.0.0.1:1234")
	checkError(err)
	for{
		if conn, err := listener.Accept(); err == nil{
			//If err is nil then that means that data is available for us so we take up this data and pass it to a new goroutine
			go handleProtoClient(conn, c)
		} else{
			continue
		}
	}
}

var errInvalidVarint = errors.New("invalid varint32 encountered")

func ReadDelimited(r io.Reader, m proto.Message) (n int, err error) {
	// Per AbstractParser#parsePartialDelimitedFrom with
	// CodedInputStream#readRawVarint32.
	var headerBuf [binary.MaxVarintLen32]byte
	var bytesRead, varIntBytes int
	var messageLength uint64
	for varIntBytes == 0 { // i.e. no varint has been decoded yet.
		if bytesRead >= len(headerBuf) {
			return bytesRead, errInvalidVarint
		}
		// We have to read byte by byte here to avoid reading more bytes
		// than required. Each read byte is appended to what we have
		// read before.
		newBytesRead, err := r.Read(headerBuf[bytesRead : bytesRead+1])
		if newBytesRead == 0 {
			if err != nil {
				return bytesRead, err
			}
			// A Reader should not return (0, nil), but if it does,
			// it should be treated as no-op (according to the
			// Reader contract). So let's go on...
			continue
		}
		bytesRead += newBytesRead
		// Now present everything read so far to the varint decoder and
		// see if a varint can be decoded already.
		messageLength, varIntBytes = proto.DecodeVarint(headerBuf[:bytesRead])
	}
	fmt.Printf("Reading %d bytes\n", messageLength)
	messageBuf := make([]byte, messageLength)
	newBytesRead, err := io.ReadFull(r, messageBuf)
	bytesRead += newBytesRead
	if err != nil {
		return bytesRead, err
	}
	return bytesRead, proto.Unmarshal(messageBuf, m)
}

func handleProtoClient(conn net.Conn, c chan *dogcollar.Activity){
	fmt.Println("Connection established")
	//Close the connection when the function exits
	defer conn.Close()
	//Create a data buffer of type byte slice with capacity of 4096
	fmt.Println("Decoding Protobuf message")
	//Create an struct pointer of type ProtobufTest.TestMessage struct
	protodata := new(dogcollar.Activity)
	//Convert all the data retrieved into the ProtobufTest.TestMessage struct type
	n, err := ReadDelimited(conn, protodata)
	fmt.Printf("Read %d bytes\n", n)
	checkError(err)
	//Push the protobuf message into a channel
	c <- protodata
}

func checkError(err error){
	if err != nil {
		fmt.Fprintf(os.Stderr, "Fatal error: %s", err.Error())
		os.Exit(1)
	}
}