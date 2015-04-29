# bully

bully is a C implementation of the bully algorithm used for leader election in a distributed system. This particular implementation constructs keeps a log of the vector time stamps of each node in the election. These logs are readily interpretable with the [ShiViz](http://bestchai.bitbucket.org/shiviz/) visualization tool.

#building

After cloning the repository bully can be built by running the command                  
**make**                               

#running
Each node in the group list takes a set of arguments that specify how the program will behave at runtime the parameters are as follows

PortNumber - the port number the node will be listening on
GroupListFileName - A file containing a list of all the nodes participating denoting their host address and port number, an example with three nodes is as follows.

ex)                    
localhost 8080                         
localhost 8081                                   
remote.ugrad.cs.ubc.ca 16999                          

LogFileName - the name of the file to which the event logs will be written
TimeoutValue - The ammount of time in seconds that a node will wait before assuming a message has been lost
AYA time - the ammount of time a non-coordinator will poll the coordinator to see if it is alive
SendFailure - the probability that sending failure occurs.

The commands are to be issued in the following format

**./node portnumber grouplistFilename logfilename timeoutvalue ayatime sendfailure**

Example
To start a node on port 8080 with the grouplist specified in group.txt, and the specified log file log.txt, a timeout value of 3 seconds, aya time of 10 seconds and a send failure rate of 5% run the following.

**./node 8080 group.txt log.txt 3 10 5**

#Running Tests
To automatically generate a run a script called test.sh has been added to the repository. It automatically starts all of the nodes specified in group.txt to run on a local machine. a runtime is specified, at which point all of the logs are concatinated together to produce a ShiViz parsable document.

Example

To run an example with 10 nodes that runs for 60 seconds, with a 5 percent failure rate, a timeout value of 10 seconds, and an AYA timeout of 5 seconds the following should be run.

**./test.sh grouplist.txt 10 5 5 60**

*grouplist.txt*                     
localhost 8080                          
localhost 8081                          
localhost 8082                          
localhost 8083                          
localhost 8084                          
localhost 8085                          
localhost 8086                          
localhost 8087                          
localhost 8088                          
localhost 8089                          

after 60 seconds a file *log.txt* will automatically be constructed from the outputs of each of the nodes.

#ShiViZ

The *log.txt* created by running the test script can be pasted into [ShiViZ](http://bestchai.bitbucket.org/shiviz/), and parsed using the following regex (?\<event\>.\*)\n(?\<host\>\S\*) (?\<clock\>{.\*})
