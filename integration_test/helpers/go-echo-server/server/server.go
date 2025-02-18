/**
 *
 * (C) Copyright 2020-2021 Hewlett Packard Enterprise Development LP
 *
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */

package main

import (
	"bufio"
	"context"
	"log"
	"net"
	"os"

	"github.com/spiffe/go-spiffe/v2/spiffeid"
	"github.com/spiffe/go-spiffe/v2/spiffetls"
	"github.com/spiffe/go-spiffe/v2/spiffetls/tlsconfig"
	"github.com/spiffe/go-spiffe/v2/workloadapi"
)

const (
	socketPath = "unix:///tmp/agent.sock"
	hostname   = "0.0.0.0"
)

func main() {
	// Setup context
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Allowed SPIFFE ID
	clientID := spiffeid.Must("example.org", "myworkloadA")

	//hostname and port for server passed as argument
	serverAddress := hostname + ":" + os.Args[1]

	// Creates a TLS listener
	// The server expects the client to present an SVID with the spiffeID: 'spiffe://example.org/myworkloadA'
	//
	// An alternative when creating Listen is using `spiffetls.Listen` that uses environment variable `SPIFFE_ENDPOINT_SOCKET`
	listener, err := spiffetls.ListenWithMode(ctx, "tcp", serverAddress,
		spiffetls.MTLSServerWithSourceOptions(
			tlsconfig.AuthorizeID(clientID),
			workloadapi.WithClientOptions(workloadapi.WithAddr(socketPath)),
		))
	if err != nil {
		log.Fatalf("Unable to create TLS listener: %v", err)
	}
	defer listener.Close()

	// Handle connections
	for {
		conn, err := listener.Accept()
		if err != nil {
			go handleError(err)
		}
		go handleConnection(conn)
	}
}

func handleConnection(conn net.Conn) {
	defer conn.Close()

	// Read incoming data into buffer
	req, err := bufio.NewReader(conn).ReadString('\n')
	if err != nil {
		log.Printf("Error reading incoming data: %v", err)
		return
	}
	log.Printf("Received from client: %q", req)

	// Send a response back to the client
	if _, err = conn.Write([]byte(req + "\n")); err != nil {
		log.Printf("Unable to send response: %v", err)
		return
	}
}

func handleError(err error) {
	log.Printf("Unable to accept connection: %v", err)
}
