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

## 1. Echo Client-Server

This was straightforward enough, although for me it was a first real primer with C

### Design


Main challenges were:
* Understanding that `recv`, `read` and others block unless explicitly configured not
to. This caused some confusion with programs hanging until I looked at the man page more plosely. Also given that you have to figure out what actually constitutes the end
of a particular socket write for example. I.e. Client 1 could be sending 15 bytes every minute vs Client 2 which could send all of the 256 bytes it wants to send at once. Thus Client 1 neccesitates, on the the other end, you read from the socket in chunks, terminating upon some pre-agreed upon delimiter like `\0` for example or simply when Client 1 closes the connection. The latter is the semantics
that held through throughout this project, although there were instances throughout this project where we could not assume the client would close the connection. Thus the server could hang indefinitely. Client 2 works under the same principle.
You could read the entire payload in 1 fell swoop, but this is probably least correct
as you assume that the client will send the payload all at once at that it will be
no larger than whatever buffer you're reading it all into. This got me in `gf_server.c` in part 3 of the assignment since I assumed that since the GETFILE protocol request is so short in simple, it'd be easiest programmatically to just assume the whole thing would be sent at once. Fixing this passed ~6/10 of the then failing test cases

## 2. Transferring a File

## 3. Implementing the Getfile Protocol

## 4. Implementing a Multithreaded Getfile Server


## Improvement Suggestions

* I enjoyed this assignment quite a bit. But what almost was not only enough to offset all of that but also make me hate it, was Bonnie and the general testing situation. You _cannot_ make the working `gfserver_main` and `gfclient_download` binaries you distribute (in the interop thread on Piazza) not reproduce the conditions for at least some of the ridiculously finicky test cases for `gfserver` and particularly `gfclient` _and then_ make it so that your only _proper_ test bed is Bonnie itself _and then also_ limit the # of submissions to 10 per day _aaaaand then_ make it so that students have to read gnarled Python trace backs and mangled logs if the failure output was gracious enough to provide them straight out of some terribly inadequate JSON Bonnie output. Seriously tell me what is the purpose of flagellating students in this way? I'm actually interested in hearing an argument/explanation for why this is the way it is, if there even is one. The whole point of this assignment lest we forget is to get comfortable with POSIX threading APIs, socket programming, and general low level programmatic concerns like managing your own memory correctly.
The most egregious example of this was the client hung issues that came up time and time again for students. While I appreciate the one thread explaining the common causes and why they happen, students like me continued to be befuzzled by whats happening after ostensibly verrifying that none of those common cases were whats happening in their code. So literally what can we do? Create a bunch of our own test cases and see if the issue magically manifests itself. And for me at least nothing came of that. I continued to try to squint at this:

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
    },
    ```
and figure out what in the hell this means. Which what really put the cherry on top of the frustration, is the fact that I'm a software engineer by profession and it would just simply never be the case, ever, conceivably, that this would be the only thing I'd have to go off of when tracking down a bug, for example. So what is my suggestion? Simply provide the test binary that logs sufficiently noisely to where you're not making it obvious but you're also not ^ not outputting logs when certain failures occur vs others, to us as a file in each folder of this project. We can test against it as much as we want and when we're done we simply submit a zip of our project or we submit to Bonnie which doesnt run anymore tests or what have you