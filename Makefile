C       = g++
LIB     =  -I/usr/csshare/src/csim_cpp-19.0/lib
CSIM    = /usr/csshare/src/csim_cpp-19.0/lib/csim.cpp.a
RM      = /bin/rm -f
CFLAG   = -DCPP -DGPP
 
INPUT = assn1_skeleton1.cpp
TARGET = csim
  
$(TARGET): $(INPUT)
	$(C) $(CFLAG) $(LIB) $(INPUT) $(CSIM) -lm -o $(TARGET)
 
clean:
	$(RM) *.o *.output core *~ *# $(TARGET)
