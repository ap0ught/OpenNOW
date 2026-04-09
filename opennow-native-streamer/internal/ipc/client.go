package ipc

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strings"
	"sync"

	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/pkg/protocol"
)

type Handler interface {
	Handle(context.Context, protocol.Envelope) error
}

type Client struct {
	conn   net.Conn
	reader *bufio.Reader
	writer *bufio.Writer
	mu     sync.Mutex
}

func Connect(ctx context.Context, endpoint string) (*Client, error) {
	dialer := &net.Dialer{}
	network := "unix"
	address := endpoint
	switch {
	case strings.HasPrefix(endpoint, "unix:"):
		address = strings.TrimPrefix(endpoint, "unix:")
	case strings.HasPrefix(endpoint, "tcp:"):
		network = "tcp"
		address = strings.TrimPrefix(endpoint, "tcp:")
	}
	conn, err := dialer.DialContext(ctx, network, address)
	if err != nil {
		return nil, err
	}
	return &Client{conn: conn, reader: bufio.NewReader(conn), writer: bufio.NewWriter(conn)}, nil
}

func (c *Client) Send(msgType string, payload any) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	body, err := json.Marshal(payload)
	if err != nil {
		return err
	}
	env, err := json.Marshal(protocol.Envelope{Type: msgType, Version: protocol.Version, Payload: body})
	if err != nil {
		return err
	}
	if _, err = c.writer.Write(append(env, '\n')); err != nil {
		return err
	}
	return c.writer.Flush()
}

func (c *Client) Run(ctx context.Context, handler Handler) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}
		line, err := c.reader.ReadBytes('\n')
		if err != nil {
			return err
		}
		var env protocol.Envelope
		if err := json.Unmarshal(line, &env); err != nil {
			return fmt.Errorf("decode envelope: %w", err)
		}
		if err := handler.Handle(ctx, env); err != nil {
			return err
		}
	}
}

func EndpointFromEnv() (string, error) {
	endpoint := os.Getenv("OPENNOW_NATIVE_STREAMER_ENDPOINT")
	if endpoint == "" {
		return "", fmt.Errorf("OPENNOW_NATIVE_STREAMER_ENDPOINT is required")
	}
	return endpoint, nil
}

func (c *Client) Close() error {
	if c.conn != nil {
		return c.conn.Close()
	}
	return nil
}
