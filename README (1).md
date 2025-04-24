omribaum, idoyanay1230
Omri Baum (315216853), Ido Yanay (208494443)

Exercise 2

FILES:

ANSWERS:

Part 1:
Q1 - a:sigsetjmp(sugjmp_buf env, int savesigs) - save the stack context and the CPU current state in the variable env.
if savesigs == 1 the currentky masked signals are saved as well.
siglongjmp(sugjmp_buf env, int val) - jumps to the CPU state and stack context kept in env. if any masked signals were saved,
it will restore it as well. ???the return value is defined by the user???

Q1 - b: sigsetjmp will save the makes signals if indicated to do so. and siglongjmp will restore it.
we conclude that masked signals can be restores and saved by those 2 functions. 

Q2: the general use we chose - simple text editor or notes editor.
user-level threads are a reasonable choice for your example because "jumping' bewtween notes will consider as user level threads transitions
and they require less then kernel level threads. for every note or file we will create a user level thread. moreover, all user level threads share the same memory space so data can be shared between notes faster than kernel level threads.

Q3: advantages:      
            1. security - if one thread has been attacked, other threads will not be affected by the attack.
               each process has its own memory space so the attacker exposed to the specific space he attacked. 
            2. stability - if one process is unresponsive or it has crashed the other processes will not be affected.
            3. resources management- memory leaks are prevented widely. if one process leaks it stays restricted to the specific process.
               furthurmore, closing a tab will invoke the OS to reclaims immediately the memory allocated for the tab.   
                     
    disadvantages:   
            1. resources use - each process gets its own memory space. and shared data may be stored multiple times
            2. overhead time - multiple processes increases the resources consumption and CPU process time. 
            3. if theres not enough free memory to create another process, opening a new tab will raise an error. 

Q4: thats the text printed in the shell by the command kill pid:

 ~~~~~~~enter here the text from kill pid ~~~~~~~~~~~`
 
 The process begins when the user types kill pid and presses the Enter key.
Each keypress generates a hardware interrupt, which is handled by the keyboard hardware and the operating system.
The OS receives the typed characters and passes them to the shell, which collects them into a string representing a command.
Once Enter is pressed, the shell parses the command (kill pid) and recognizes it as a request to send a signal to a specific process.
The shell then creates a system call to the operating system, asking it to send the SIGTERM signal to the process with the given PID.
The operating system receives the system call, identifies the specific process based on the PID, and sends it the SIGTERM signal.


Q5:In real time data is being processed immediately and calculations are executed immediately too. in general, the timing 
constraints must be met. while virtual time is the time the process\thread "lives" in. it can be stopped and lately continued from the poiny where it stopped.
the time responses to certain conditions or events but real time keeps ticking constatly. an example for using real time is traffic light mangment system.
it obviously can not "freeze" becuase the traffic keeps moving.  
