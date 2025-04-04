# Compiler and flags
CC = gcc
CFLAGS = -Wall -g  # Enable debugging symbols
SERVER = tcp_server
CLIENT = tcp_client
PORT = 12345

# Targets

all: namespace_setup $(SERVER) $(CLIENT)

# Step 1: Run the namespace setup script
namespace_setup:
	@echo "Setting up network namespaces and veth interfaces..."
	sudo bash namespace_veth_bridge.sh create

# Step 2: Compile the server and client
$(SERVER): tcp_server.c
	@echo "Compiling the TCP server..."
	$(CC) $(CFLAGS) -o $(SERVER) tcp_server.c

$(CLIENT): tcp_client.c
	@echo "Compiling the TCP client..."
	$(CC) $(CFLAGS) -o $(CLIENT) tcp_client.c

# Step 3: Run the server and client and collect metrics
run: all
	@echo "Running the TCP server and client..."
	# Run server in background (in NS1)
	sudo ip netns exec NS3 ./$(SERVER) &  # Run server in NS1
	@echo "Server started in NS1."

	# Run client in NS3 (or NS2, depending on your testing setup)
	sudo ip netns exec NS1 ./$(CLIENT)  # Run client in NS3 (or NS2)

	@echo "Client finished execution. Metrics can now be reviewed."

# Step 4: Clean up (delete namespaces and generated files)
clean:
	@echo "Cleaning up network namespaces and veth interfaces..."
	rm -f $(SERVER) $(CLIENT)
	sudo bash namespace_veth_bridge.sh cleanup
