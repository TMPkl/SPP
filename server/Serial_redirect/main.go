package main

import (
	"bufio"
	"fmt"
	"net"
)

func handle(conn net.Conn) {
	defer conn.Close()

	addr := conn.RemoteAddr().String()
	fmt.Println("Connected:", addr)

	reader := bufio.NewReader(conn)

	id, err := reader.ReadString('\n')
	if err != nil {
		fmt.Println("Failed to read ID:", err)
		return
	}

	id = id[:len(id)-1]
	fmt.Println("Device ID:", id)

	// 2. Czytaj logi
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			fmt.Printf("Disconnected: %s (%s)\n", id, addr)
			return
		}

		fmt.Printf("[%s] %s", id, line)
	}
}

func main() {
	listener, err := net.Listen("tcp", ":6110")
	if err != nil {
		panic(err)
	}

	fmt.Println("Server listening on :6110")

	for {
		conn, err := listener.Accept()
		if err != nil {
			continue
		}

		go handle(conn)
	}
}
