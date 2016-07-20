/*
 * exception.h
 *
 *  Created on: Jul 10, 2016
 *      Author: root
 */

#ifndef EXCEPTION_H_
#define EXCEPTION_H_

// names are unique within a function, any valid variable name
// a jump table try, allowing return values to determine the exception thrown(index in a jump table, -1 being success for example)
// you only need to put in what you need, not all exceptions, or total exception count
#define GTRY(name, exc) void* __jt_##name[exc];int __jtix_##name = 0;
// register an exception in the jump table. you don't need to do them all
#define GTEX(name, exception) __jtix_##name;__jt_##name[__jtix_##name++] = &&__tr_##exception##name;
// resets the jump table index to a given value, useful for overwriting exceptions without a new try.
#define GTRST(name, index) __jtix_##name = index;
// a standard try, doesn't actually add anything, but for cleanliness.
#define TRY(name) ;
// throw an exception by name
#define THROW(name, exception) goto __tr_##exception##name;
// throws an exception by jump table index
#define GTHROW(name, exn) goto *__jt_##name[exn];
// begins a catch block
#define CATCH(name, exception) goto __trend_##name;__tr_##exception##name:;
// begins a catch continuation, to be put directly after a catch.
#define CCATCH(name, exception) __tr_##exception##name:;
// signifies where the try ends
#define ENDTRY(name) __trend_##name:;
// 'finally' blocks need only be put after an end try.

#endif /* EXCEPTION_H_ */
