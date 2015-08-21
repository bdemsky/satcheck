Contains a Clang extension which generates MC2 calls based on calls already in the code.

* To compile this code, you need to modify the build paths in
the Makefile; point them at your LLVM build. Then running 'make' should work.

* To run the code, you can specify the include directories explicitly:

build/add_mc2_annotations ../test/userprog_unannotated.c -- -I/usr/include -I../../llvm/tools/clang/lib/Headers -I../include

I can't figure out how to make Clang's compilation database work for me.

* Currently, the tool will fail if it tries to rewrite any code that uses macros.
One example of a macro is "bool".

The following StackOverflow link explains how to overcome that problem.

http://stackoverflow.com/questions/24062989/clang-fails-replacing-a-statement-if-it-contains-a-macro

It's on the TODO list.

* The tool won't deal properly with a malloc hidden behind a cast,
e.g. (struct node *) malloc(sizeof(struct node *)). You don't need to
cast the malloc. Ignoring the cast is on the TODO list, but not done yet.

* Naked loads aren't allowed, e.g. Load_32(&x) without y = Load....

This is especially relevant in the context of a loop, e.g. while (Load_32(&x)).

>    for (int i = 0; i < PROBLEMSIZE; i++)
>        r1 = seqlock_read();

* for loops with a non-CompoundStmt body can generate wrong code
* if you throw away the return value (just "seqlock_read()"), then the rewriter won't work right either


linuxlock, linuxrwlock.
* I changed the global 'mylock' variable so that it's allocated on the heap, not
as a global variable.
* Should I look into the fact that linuxlock now generates nextOpStoreOffset rather than nextOpStore
as in the golden master? That is probably actually correct.

-----

1. variables of union type which are allocated as global variable and then nextOpStoreOffset'd cause a problem
2. boolean types are handled as macros and so there's no source position
3. if statements with conds like "if (write_trylock(mylock))" don't work.
4. malloc on vardecl (e.g. struct node * node_ptr = (struct node *)malloc) don't always get mc2_function generated for them [ms-queue-simple]
5. nextOpLoadOffset not generated in following case:

               MCID _fn1; struct node ** tail_ptr_next= & tail->next;
               _fn1 = MC2_function_id(2, 1, sizeof (tail_ptr_next), (uint64_t)tail_ptr_next, _mtail);
 
               MCID _mnext; _mnext=MC2_nextOpLoadOffset(_fn1, MC2_OFFSET(struct node *, next)); struct node * next = (struct node *) load_64(tail_ptr_next);

currently we generate nextOpLoad.
