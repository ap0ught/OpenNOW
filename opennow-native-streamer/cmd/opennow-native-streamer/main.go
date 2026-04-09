package main

import (
	"context"
	"os/signal"
	"syscall"

	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/control"
	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/ipc"
	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/logging"
)

func main() {
	logger := logging.New("[native-streamer]")
	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	endpoint, err := ipc.EndpointFromEnv()
	if err != nil {
		logger.Fatal(err)
	}
	client, err := ipc.Connect(ctx, endpoint)
	if err != nil {
		logger.Fatal(err)
	}
	service := control.New(client)
	defer func() { _ = service.Close() }()
	if err := service.Hello(ctx); err != nil {
		logger.Fatal(err)
	}
	if err := client.Run(ctx, service); err != nil && ctx.Err() == nil {
		logger.Fatal(err)
	}
}
