### Documentation for **Part 2: Exploring Network Namespaces and Kernel Bypass Techniques**

### **Network Namespace and veth Interface Setup**

In this setup, three namespaces (`NS1`, `NS2`, and `NS3`) are created, and network interfaces are configured between them:
- **NS1** and **NS3** each have one veth interface.
- **NS2** is configured as a **bridge** connecting **NS1** and **NS3**.
- The `namespace_veth_bridge.sh` script automates the creation of namespaces and the veth interface setup.

**Simulation of Network Conditions**:
- The **tc** command introduces a **0.1ms delay** and **1% packet loss** on the veth interface in **NS3** to simulate real-world network conditions.

---

### **Case 1 - Traditional TCP Client-Server (Using Linux Sockets)**

In **Case 1**, the **TCP client** (`tcp_client.c`) runs in **NS1**, and the **TCP server** (`tcp_server.c`) runs in **NS3**. The client sends **random-sized packets** (between 512 bytes and 1024 bytes) to the server, which responds with the **length** of the received packet.

#### **Makefile (Case 1)**

The **Makefile** automates the following steps:
1. **Namespace Setup**: The `namespace_veth_bridge.sh` script is called to create the network namespaces and configure the veth interfaces.
2. **Compilation**: Compiles the TCP client (`tcp_client.c`) and server (`tcp_server.c`).
3. **Execution**: Runs the server in **NS3** and the client in **NS1**, collecting the metrics such as latency and bandwidth.

#### **Results - Case 1** (Traditional TCP over Linux Sockets)

The results of the experiment with the traditional **Linux TCP client-server** implementation (over **10000**, **100000**, and **1000000** packets) are as follows:

| **Packets** | **Latency**   | **Bandwidth** |
|-------------|---------------|---------------|
| **10,000**  | **0.002 s**  | **311 kB/s**  |
| **100,000** | **0.00024 s**| **381 kB/s**  |
| **1,000,000** | **0.00028 s** | **410 kB/s** |

- **Latency** is quite low, even with a larger number of packets, indicating a relatively fast round-trip time, which is typical of local machine communication.
- **Bandwidth** increases slightly as the number of packets increases, with **1,000,000** packets achieving a bandwidth of **410 kB/s**.

---

### **Case 2 - TCP Server Using DPDK and F-Stack (Kernel Bypass)**

In **Case 2**, the server is implemented using **DPDK** (Data Plane Development Kit) and **F-Stack** for **user-space networking**, bypassing the kernel networking stack. The DPDK server uses **F-Stack** to manage **TCP/IP** in user space.

#### **Makefile (Case 2)**

The **Makefile** in **Case 2** handles the following:
1. **Setup**: The `setup.sh` script configures DPDK, including:
   - Allocating huge pages for DPDK.
   - Loading necessary kernel modules (`uio`, `igb_uio`, `rte_kni`).
   - Binding the NIC to **DPDK** using the `dpdk-devbind.py` tool.
2. **Compilation**: Compiles the DPDK client and server programs (`dpdk_client.c`, `dpdk_server.c`).
3. **Execution**: Runs the **DPDK server** and **client**. The server runs using the configuration file `config.ini`, and the client sends packets to the server.

#### **Results - Case 2** (TCP Server using DPDK and F-Stack)

The experiment in **Case 2** is conducted on **different machines** using **virtual NICs** in a **virtual machine** environment. The results show **significant improvements** in both **latency** and **bandwidth** compared to Case 1:

| **Packets** | **Latency**   | **Bandwidth**   |
|-------------|---------------|-----------------|
| **10,000**  | **0.0001 s** | **7476 kb/s** |
| **100,000** | **0.0001 ms**| **7504 kb/s** |
| **1,000,000** | **0.000099 s** | **7574 kb/s** |

- **Latency** is significantly lower (in microseconds) in **Case 2** due to **DPDK's kernel bypassing** and optimized packet processing.
- **Bandwidth** is also substantially higher, with **1,000,000** packets achieving **12,400 kb/s**, which is a significant improvement over **Case 1**.

The performance gains in **Case 2** are due to the **F-Stack** and **DPDK**’s ability to process packets directly in user space, eliminating the overhead of kernel processing and providing high throughput and low-latency networking.

---

### **Setup for DPDK and F-Stack in Case 2**

To run **Case 2**, the following setup is required:

1. **Virtual NIC Setup**:
   - In the **virtual machine** environment, configure **virtual NICs** for DPDK. This allows the VM to emulate physical network interfaces and enables DPDK to manage network traffic in user space with minimal kernel interference.
   
2. **Hugepage Memory Allocation**:
   - Allocate **hugepages** for **DPDK** by writing the value `1024` to `/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`.

3. **Mounting hugetlbfs**:
   - Create a mount point for `hugetlbfs` and mount it at `/mnt/huge`.

4. **Loading Kernel Modules**:
   - Load the required kernel modules for DPDK:
     - `uio` for user-space I/O.
     - `igb_uio` for Intel NIC support.
     - `rte_kni` for kernel NIC interface support with carrier set to `on`.

5. **DPDK Device Binding**:
   - Use the `dpdk-devbind.py` tool to bind the NIC (`enp2s0`) to the **DPDK driver** (`igb_uio`).

6. **DPDK and F-Stack Setup**:
   - Clone the **F-Stack** repository and compile it.
   - Ensure that the **DPDK** libraries and **F-Stack** are correctly configured in the server’s `config.ini` file.
