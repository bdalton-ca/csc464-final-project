Problem/Question
	My initial plan was to create a sort of distributed chatbot program which
	would share state across many users. This was inspired by Internet Relay Chat,
	because standard IRC protocols don't provide a chat log; the chat
	history is not recorded, and if a user is not logged in, they will have to
	depend on a user being online who can provide them with the log.
	One way this problem is solved is through the use of utility bots, which,
	unlike a conversational chatbot, simply responds to commands.
	These chatbots can be programmed to perform various tasks such as 
	sending messages to users who log in, serving media, and playing games.
	Unfortunately, these chatbots must be hosted by a user, and when this
	user is offline, the chatbot will also be offline.
	
Description

	The code contained in this repository implements a distributed hash table using the 
	Kademlia protocol. It is a relatively simple system, consisting of nodes containing 
	partial hash tables which can communicate with eachother to look up entries.
	Nodes are organized in a way where they can quickly find a node which contains the
	data for a certain hash. This is done ensuring that data is stored in nodes which
	have an ID which is as close as possible to the hash being looked up.
	
	Watch this video for a more detailed description of the kademlia protocol, with diagrams:
		https://www.youtube.com/watch?v=DENgiwS5NFs&feature=youtu.be
	
	A chat client application has been developed which uses this DHT code.
	The client is not fully integrated with the DHT, so it can only do basic message
	sending along with a couple of commands to interact with peer hash tables.
	It represents a simple prototype of a chat system that could be used to store
	data such as image files and logs without relying on a central server.
	
Evaluation

	The evaluation of this project is largely dependent on correctness.
	To test the DHT code the algorithms were first written and tested in 
	a non networked simulation. Instances of nodes were created and a variety of tests
	were run to ensure the system worked properly. 
	First routing table population was tested to ensure that the nodes could communicate
	as the specification requires. This is simply a matter of sending messages throughout
	the network and checking the tables have the correct properties. (Bucket size and coverage.)
	Next with these connected nodes the actual search and store operations of Kademlia
	were tested with different sets of data to ensure that the communcations protocol worked.
	
	In the DHT directory you will find the DHT implementation and some tests results.
	1000 hashed pieces of data were generated and stored by random nodes in the network,
	and were then retrieved by random nodes in the network. In the results in out.txt all 1000
	pieces of data could successfully be found in around five or ten queries of the DHT.
	There were found to be some cases where data could not be found, but this is likely due 
	to certain nodes not having well populated routing tables. This is something in particular
	that could be improved. However, in general the initial routing table generation
	should be smoothed out over time as the network matures.
	
	This test shows the correctness of the algorithm by fuzzing the input space.
	The random node choices and random pieces of data combined with both storing and retrieving
	operation cover all the operations of the kademlia protocol involving searching, node hopping,
	routing table updating and so on. The results show that the algorithm is working as intended
	and can handle a load of random data in a simple context.

Future Work

	The main work going forward is of course finishing the integration of the DHT into
	the client program. Currently the PING and STORE remote procedure calls are working,
	but there are issues with some of the networking code for the FIND_NODE and FIND_VALUE calls.

	The code currently is able to run on applications running on the same host on different ports.
	This is a test implementation. Once the client is fully operational, it should be simple to switch
	these port connection to fully networked connections.
	
	There are also some parts of Kademlia which are not implemented for the sake of simplicity.
	There are no expiration and republishing timeouts; data will stay in the hash table indefinitely.
	This would of course be a problem for a large scale application, but for the prototyping stage
	it isn't exactly necessary. Adding the timeouts and conforming to the rest of the specification
	should be relatively trivial once the networking is working.