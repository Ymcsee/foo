# Project README file

This is **YOUR** Readme file.

**Note** - if you prefer, you may submit a PDF version of this readme file.  Simply create a file called **readme-student.pdf** in the same directory.  When you submit, one or the other (or both) of these files will be submitted.  This is entirely optional.

**Note: the `submit.py` script will only submit files named readme-student.md and readme-student.pdf**

## Project Description

Your README file is your opportunity to demonstrate to us that you understand the project.  Ideally, this
should be a guide that someone not familiar with the project could pick up and read and understand
what you did, and why you did it.

Specifically, we will evaluate your submission based upon:

- Your project design.  Pictures are particularly helpful here.
- Your explanation of the trade-offs that you considered, the choices you made, and _why_ you made those choices.
- A description of the flow of control within your submission. Pictures are helpful here.
- How you implemented your code. This should be a high level description, not a rehash of your code.
- How you _tested_ your code.  Remember, Bonnie isn't a test suite.  Tell us about the tests you developed.
  Explain any tests you _used_ but did not develop.
- References: this should be every bit of code that you used but did not write.  If you copy code from
  one part of the project to another, we expect you to document it. If you _read_ anything that helped you
  in your work, tell us what it was.  If you referred to someone else's code, you should tell us here.
  Thus, this would include articles on Stack Overflow, any repositories you referenced on github.com, any
  books you used, any manual pages you consulted.


In addition, you have an opportunity to earn extra credit.  To do so, we want to see something that
adds value to the project.  Observing that there is a problem or issue _is not enough_.  We want
something that is easily actioned.  Examples of this include:

- Suggestions for additional tests, along with an explanation of _how_ you envision it being tested
- Suggested revisions to the instructions, code, comments, etc.  It's not enough to say "I found
  this confusing" - we want you to tell us what you would have said _instead_.

While we do award extra credit, we do so sparingly.

# BEGIN WRITE-UP

## 1. Echo Client-Server

This was straightforward enough, although for me it was a first real primer with C

### Main challenges were:
* Understanding the basics of setting up a socket, reading from it, and tearing it down. Any programming language I've done this in before abstracts these lower level details out for you
* Understanding that `recv`, `read` and others block unless explicitly configured not
to. This caused some confusion with programs hanging until I looked at the `man` page more closely. This didn't matter so much for this section since we're not receiving sending and receiving paylaods in chunks i.e. a single call to `recv` will do
* Encountering first Bonnie frustrations

### Echo Client

#### Flow
1. Assign socket file descriptor `sockfd` via `socket`
2. Set up `sockaddr_in`
3. Bind `sockaddr` to `sockfd` via `connect` 
4. Send the `message` assigned from cmd line arg via `send` to `sockfd`
5. Use `recv` to receive response in one go into a char array `resp_buffer`
6. Null terminate `resp_buffer`
7. Close `sockfd` and `exit`

### Echo Server

#### Flow
1. Assign socket file descriptor `sockfd` via `socket`
2. Set up `sockaddr_in`
3. Do:
```
int optval = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
```
to satisfy Bonnie
4. Bind `sockaddr` to `sockfd` via `connect` 
5. Listen for `maxnpending` connections to `sockfd` via `listen`
6. Do forever:
7. Initialize client file descripter `client` via `accept`
8. Ensure receive buffer `message` is zero-ed out since this is running in a loop
9. `recv` the client request reading into the `message` buffer
10. Null terminate `message`
11. Send `message` back to client
12. Close `clientfd`
13. Outside of loop, close `sockfd` and exit

## 2. Transferring a File

### Main challenges were:
* Opening and reading a file
* Writing the server's response to that file

### Transfer Client

#### Flow
1. Repeat 1-3 from Echo Client
2. Open a file for reading
3. `read` (should've used `recv` as its specific to sockets but `read` works fine) the response from the server in a `while` loop i.e. in chunks
4. `fwrite` those chunks to the file descriptor we initalized in 2
5. Close the socket and file descriptor and exit

### Transfer Server

#### Flow
1. Open the file at the path specified in the cmd line param
2. Repeat 1-7 from Echo Server
3. In a while loop, use `fread` to read from that file into the response buffer directly in chunks
4. Send each chunk to `clientfd` and zero out response buffer before the next iteration
5. Close connection and repeat forever
6. Outside the while loop, close the server socket and the file

## 3. Implementing the Getfile Protocol

### Main challenges were:
* Bonnie frustrations came to a head with respect to GFClient
* GFServer was straightforward in comparison
* It was critically important to break out of while loops on time here i.e. having cogent terminating conditions, before potentially blocking forever at another call to `recv`
* All request and responses had to be read into chunks. For example initially i wasn't doing so in GFServer assuming that because the request was so short a client would surely send it all in one chunk. But this doesn't have to be the case at all and its wrong to assume so. Once I fixed this it passed 5/6 failing Bonnie test cases
* Figuring out `regex.h` was harder than expected due to lack of documentation but the final approach was much cleaner and more robust than using `strstr` and/or `strtok` repetively and manually validating the request and response headers imo
* Making sure to free memory correctly -- didn't end up getting this one 100%

### GFClient

#### Flow
1. We do the typical routine to set up the connection
2. Craft the request: `sprintf(request, header_template, (*gfr)->path);`
3. Receive the header. Since this can be sent in arbitrary chunks we
`recv` until `strstr(buffer, "\r\n\r\n") != NULL`. Of course this can result
in the client hanging when the response header from the server is completely invalid
for example (since we may never see a `\r\n\r\n`) so its important to verify that 
the header remains valid as you're reading it
4. Verify the header is even valid using regex and parse its status into `gfr`. If it is and its status is `OK` we parse the file length into `gfr` as well. Otherwise if the header is invalid we can exit early. If it's status is not `OK` we can invoke the header callback if its not `NULL` and exit early.
5. If the write callback on `gfr` is not `NULL` we proceed to receive the actual content of the response. We can split the response we've read so far along the delimiter `\r\n\r\n` and write the second half which is the initial content into `content_buffer` and set `bytesreceived` on `gfr` initially to its length
6. From there we read the rest of the response in chunks in a while loop until `bytesreceived` on `gfr` equals the file length we parsed out of the header
7. At each iteration we invoke the write callback and increment `bytesreceived`
8. Clean up and exit

### GFServer

#### Flow
1. Typical routine to set up the socket, listen, and accept connections
2. We `malloc` a `gfcontext_t` in which to store the request scoped data we care about
3. Read the request in chunks in a while loop until the response we've read in contains the delimiter `\r\n\r\n` or `bytes_receieved > MAX_REQUEST_LEN`
4. Use regex to validate the request and parse the path to the file. If the request is invalid we send `GETFILE INVALID` back to the client and abort
5. Invoke the `handler` function on `gfserver_t **gfs`
6. Repeat forever

## 4. Implementing a Multithreaded Getfile Server

### Main challenges were:
* Race conditions and ensuring memory was freed at the right time
* Ensuring you understanding the semantics of 
  * `pthread_mutex_lock`/`pthread_mutext_unlock`
  * `pthread_cond_wait`/`pthread_cond_signal`/`pthread_cond_broadcast`
* Fork and join the threads on the thread pool correctly
* Figure out what a steque is

### GFClient MT

#### Flow
1. Initialized the `nthreads` member thread pool and start the threads
2. In each worker thread, we enter an infinite loop and wait until the steque is non empty via a `pthread_cond_wait`
3. For each `nrequests`, generate a file path via `workload_get_path()` and then
acquire the lock on the steque, enqueue the file path, signal to the consumer that
a work item has been enqueue and release the lock
3. A worker wakes up via the signal and emerges out of its wait condition, acquiring the steque lock, pulls a path off of the steque, and releases the lock
4. From here the initially provided template logic proceeds i.e. the request is crafted and sent and the status and number of bytes recieved is printed to stdout
5. Acquire the lock guarding `reqs_left` and decrement it, signal to the `main_cond` (controls the main thread waiting), and release the lock
6. In the meantime after enqueue all the work on the steque, the main thread hits a `pthread_cond_wait` itself (on said `main_cond`) and when a worker completes step 5 it emerges from its wait and checks if `reqs_left > 0` and if so it goes back to sleep. Otherwise if all the requests have been processed it does a `pthread_cond_broadcast` to any workers that are still asleep.
7. They see that `reqs_left` is `0` as well and they break out of the infinite loop an return
8. The main thread does a `pthread_join` on all th workers in a loop
9. Global cleanup


### GFServer MT

#### Flow
1. Initialized the `nthreads` member thread pool and start the threads
2. In each worker thread, we enter an infinite loop and wait until the steque has a work `queue_item` in it
3. The server is started and we're ready to field requests
4. When a request comes in and invokes `gfs_handler`, we create a `queue_item`, populate it with the request path and `gfcontext_t *`, acquire the steque lock, add it to the steque, signal to the workers, release the lock, `*ctx = NULL`, and return `gfh_success`
5. This awakens a worker which acquires the steque lock, pulls a `queue_item` off of it, and releases the lock
6. The worker uses `content_get` on the request path to initialize the file descriptor to the file to send back. 
7. Use `fstat` to get the file size
8. Send a header with status `OK` and the file length back to the client
9. Read the file in chunks via `pread` into `resp_buffer` and call `gfs_send` on each chunk until `pread` returns 0
10. Call `free` on the `queue_item`
11. Repeat forever 


## Testing
* I honestly didn't write very many test cases. Only for Bonnie failures that I had a hunch as to what was causing them. As far as that goes I was only stuck to any extent on GFClient and GFServer (GFClient was excruciating). So for those I populated my own `content.txt` and files. Beyond that just running the out of the box compiled binaries, observing the output, and using print statements to debug things was enough to get things to a submittable state. And then from there, I read the output of each remaining failing Bonnie test to further iron out edge cases

## Improvement Suggestions

* Thank you for the deadline extension
* I enjoyed this assignment quite a bit. But what almost was not only enough to offset all of that but also ruin all of it, was Bonnie and the general testing situation. Specifically as it manifested with `gfclient`. In practice you can obviously do whatever you want, it's a free country and it's your class after all, but imo, you _shouldn't_ make the working `gfserver_main` and `gfclient_download` binaries you distribute (in the interop thread on Piazza) not reproduce the conditions for at least some of the ridiculously finicky test cases for `gfclient` _and then_ make it so that your only _proper_ test bed is Bonnie itself _and then also_ limit the # of submissions to 10 per day _aaaaand then_ make it so that students have to read gnarled Python tracebacks and mangled logs, if the failure output was gracious enough to provide them (for GFClient for example different runs on the same code would produce different results, which doesn't make a fantastic amount of sense), straight out of some terribly inadequate JSON Bonnie output. There is no purpose in flagellating students in this way. But if you have a valid reason for why this is the way it is, I'm genuinely interested in hearing it. The whole point of this assignment lest we forget is to get comfortable with POSIX threading APIs, socket programming, and general low level programmatic concerns like managing your own memory correctly.
The most egregious example of this was the client hung issues that came up time and time again for students. While I appreciate the one thread explaining the common causes and why they happen, students like me continued to be befuzzled by whats happening after ostensibly verrifying that none of those common cases were whats happening in their code. And of course subsequent student responses to questions like were so totally vague with respect to what the actual issue was. I suspect for fear of giving it away. So what can we do? Create a bunch of our own test cases and see if the issue magically manifests itself. And for me at least nothing came of that. I continued to try to squint at this:

```
{
  "output": {
    "client_returncode": null,
    "server_console": "",
    "passfail": "failed",
    "server_returncode": null,
    "client_console": ""
  },
  "traceback": "Traceback (most recent call last):\n  File \"/home/grader/pr1_gfclient/gios.py\", line 293, in func_wrapper\n    ans = func(self)\n  File \"run.py\", line 39, in test_ok_with_short_message\n    ['./bvtgfclient_main', '-p', self.port], client_preload_log='gios_test1.dat')\n  File \"/home/grader/pr1_gfclient/gios.py\", line 231, in converse\n    self.assertIsNotNone(self.p_client.poll(), \"The client is taking too long (probably hung).\")\nAssertionError: The client is taking too long (probably hung).\n",
  "description": "Tests that the client properly handles an OK response and a short message (less than 1000 bytes)"
},
{
  "output": {
    "client_returncode": null,
    "server_console": "",
    "passfail": "failed",
    "server_returncode": null,
    "client_console": ""
  },
  "traceback": "Traceback (most recent call last):\n  File \"/home/grader/pr1_gfclient/gios.py\", line 293, in func_wrapper\n    ans = func(self)\n  File \"run.py\", line 211, in test_special_file_content\n    ['./bvtgfclient_main', '-p', self.port, '-c', '1'])\n  File \"/home/grader/pr1_gfclient/gios.py\", line 231, in converse\n    self.assertIsNotNone(self.p_client.poll(), \"The client is taking too long (probably hung).\")\nAssertionError: The client is taking too long (probably hung).\n",
  "description": "Tests to ensure client can properly handle a file with control data"
}
```

and figure out what the hell this means, like "control data" or "short message". I former would pass occasionally. The latter I don't think I ever got to pass, I anticipate it had something to do with the file length being 512 and me using `BUFSIZ` which is of length 1024 and losing some one or 2 bytes along the way. The frustration was really compounded by the fact that I'm a software engineer by profession and it would just simply never be the case, ever, conceivably, that this would be the only thing I'd have to go off of when tracking down a bug, for example. So anyway my suggestion is to simply provide the test binary, that logs sufficiently noisely to where you're not making it obvious but you're also not leaving students with that ^, to us as a file in each folder of this project. We can test against it as much as we want and when we're done we simply submit a zip of our project or we submit to Bonnie which doesnt run anymore tests or what have you. Along the way students will express frustration around noise they're not seeing or noise they are but don't seem to find valuable so as with anything you can adjust whats logged and what isn't in the test suite as you go. Or just use your existing test suite but provide the binary. Basically Bonnie with a submission limit _and_ esoteric test cases that are enormously finicky and nothing about the class or assignment has illuminated what could be causing the failures so students have to troll Piazza and scrounge and wait for answers to their questions, is silly and backwards and teaches students nothing about operating systems and other course topics
* Having 10 submissions per day + saying the final submission will be the one that counts turns this into a video game. Again silly and unnecessary. Example: my last GFClient submission has 4 test failures because I thought my submissions would reset at midnight so I was spamming Bonnie trying to do things to get the 2-3 tests that were failing before to pass since I thought my quota was about to reset. Now one more is failing and I didn't get a chance to clean my code. I enjoy video games but only when fake character lives are at stake, not things I actually care about like my grade. Again what does this have to do with operating systems?
* Beyond that the assignment was great. Thank you!