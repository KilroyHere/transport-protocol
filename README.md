# CS118 Project 2

### **Project Members:** 
* Abhishek Marda : 205388066
* Aryan Patel : 005329756 
* Sanchit Agarwal : 105389151 


## **Contributions:**
> **Abhishek Marda**
Server and client functions, debugging, documentation, and design.

> **Aryan Patel**
Server and client functions, debugging, documentation, and design.

> **Sanchit Agarwal**
Server and client functions, debugging, documentation, and design. 

## **Server Implementation**
### **Pseudocode**
**The Server's overall psudocodde can be thought of as follows:**
> * Initialize the server by seting up the socket 
> * Check and close any timed out connection every iteration
>     * Check the Connection Timeout Timer (10) for every connection and close  the connection if the timer has lapsed
>     * Also check the retransmission timer for the FIN packet sent to any of the connections, if the the timer has lapsed, retransmit the FIN Packet
> * In a Forever Running Loop:
>     * Check if the socket has received a packet
>         * If true take the payload and create own custom TCP Packet
>     * Check if the packet just received is from a new Connection or from an existing one.
>     * If from a new connection:
>         * Provide a custom Connection ID to the packet and set up a TCP Connection Block (TCB) for the new connection and start the connection timeout timer for the new connection.
>         * Add this packet to the new TCB and open a file in the specified directory to start storing the file being received
>     * If from an existing connection:
>         * Reset the Connection Timeout timer for the packet specific connection
>         * Check if the packet is a FIN Packet and start initiating a 4 way handwave
>         * If packet is not a FIN, add the packet's payload to the connection's payload buffer,where the connection is identified by the packet's Connection ID and stores in its TCB
>         * If the packet is the same as the next expected packet, the packet's data is read and written to the local file, the packet is deleted and the buffer is shifted
>         * Print the Data of the packet to stdout
>     * An ack Packet is created in response to the latest packet received
>         * If the the next expected sequence number is updated, so is the ack number to be sent
>     * The packet has been processed by now and is deleted

### **Design**
The Server program was made into a single high level class that handled all Server related operations.
![](https://codimd.s3.shivering-isles.com/demo/uploads/d5c47b5a5cbec5190d24e5a3d.png)

The Server class stored the details of all connections using a TCP Connection Block (TCB Struct).
![](https://codimd.s3.shivering-isles.com/demo/uploads/d5c47b5a5cbec5190d24e5a3e.png)

The entrypoint to the Server was in the function handleConnection() which implemented the pseudocode described in the section above to the following:
![](https://codimd.s3.shivering-isles.com/demo/uploads/d5c47b5a5cbec5190d24e5a43.png)



## **Client Implementation**
### **Pseudocode**

**The client's overall pseudocode can be thought of as follows:**

> * Initialize sockets and attempt to resolve hostname for the server (exit if failed)
> * Initiate connection with the server by repeatedly sending a SYN with connection ID=0 and wait for SYN-ACK (exit on connection timeout)
> * Send ACK with payload to complete the three-way handshake and start sending the payload
> * Loop: 
>     * Check connection timeout
>         * If timeout then exit with code 1
>     * Check retransmission timeeout
>         * If first packet's retransmission timeout has expired then drop all packets, start reading file from the last successfully ACKed position
>     * Read any new packets
>     * If no new packets to read and all packets have been ACK'ed:
>         * break loop
>     * Add the new packets to the packet buffer 
>     * Send the new packets, start each new packet's retransmission timer
>     * While there is an ACK packet from the server:
>         * Reset the connection timeout
>         * Check if the ACK is for a packet that has been sent before, drop ACK otherwise
>         * Mark whatever packets the ACK is for as "ACKed"
>         * From the beginning, whatever packets have been ACKed, remove them and shift the rest of the packets in the buffer forward
>         * Record the above value as shiftedBytes
>         * Scale congestion control by 1 ACK and record the change in CWND as cwndChange
>         * The avaialable space to read bytes is incremented by shiftedBytes + cwndChange bytes
> * Send FIN and await FIN-ACK (exit on connection timeout)
> * Send ACK on FIN-ACK and exit


### **Design**

The client program was made into a single high level class that handled all client related operations.

<a href="https://ibb.co/KmLSrKf"><img src="https://i.ibb.co/XV3MCYK/Screen-Shot-2022-03-06-at-3-34-57-PM.png" border="0"></a>

The entrypoint to the client was in the function `run()` which implemented the pseudocode described in the section above to the following:

<a href="https://ibb.co/vmpc138"><img src="https://i.ibb.co/P4Krxc8/Screen-Shot-2022-03-06-at-3-40-27-PM.png"></a>

We utilized the helper class `TCPPacket` that we made which stores information and provides getter functions for a TCP Segment.

We also ensured to split the client into `client.hpp` and `client.cpp` to seperate class declarations and definitions respectively. The main function was included in the `client.cpp` file.

## **Problems we ran into**

This project was a challenge for us and it took upwards of 60 hrs to complete it. In the spirit of following good coding practices we spent a significant amount of time in the beginning to chart out the class design and make the overall design modular. To make collaboration easier we used git branches and even added PR rules to the main branch. To make the code more readable we used global constants and markdown commenting.

The project was challenging because of a variety of reasons and some of the problems we faced were: 
* Understanding the spec as the protocol outlined in the spec (confundo) is somewhat different from TCP which made it difficult to understand it nuances
* Wraparound of the sequence numbers was one of the major problems we faced and a lot of time was spent first on coming up with an elegant way to do wraparound and then debugging the issues that wraparound led to. Wraparound was accounted for in a variety of functions all throughout server and client such as markAck, printPacket, createPacket etc. The major problems that we ran into with wraparound were:
    * A +1 error when the next packet that the server received has a sequence number just after wraparound (this is an issue since the sequence number starts from 0)
    * Marking Acks in a case where all the packets in my buffer have a sequence number greater then the packet received but due to overlap that packet is newer as compared to the packets already in the buffer 
    * Printing dups if a packet to be sent is between the last acked sequence number and the latest sequence number 

* One of the other problems we faced was making a TCP packet in the given format as it lead to endianess issues and reading/creating bit level data.
* In the client we initially faced the problem of increasing cwnd fractionally but we later realized that we are not required to do that and can round down the cwnd to the 512 multiple. 
* We also faced the problem of receiving packets really slow because in every iteration of the main loop in client we were sending all the packets in the available window but were receiving just 1 packet. We solved that problem by receiving all the packets available in a loop. 
* One of the problems we faced in server was arranging packets in order and flushing them to the file in the same way. We solved that by having a bitVector and flushing all the contiguosly received packets to the file. 

These are some of the problems we faced. This project involved a lot of experimenting, reviews and debugging to make everything work. I would like to all the TAs and Professor Zhang for helping along the way. 