write "hello"
write "world"
s = "hello"
t = "world"
if "hello" == "world" goto L1
goto L2
L1:
write "same"
goto L3
L2:
write "different"
L3:
if s != t goto L4
goto L5
L4:
write "not equal"
L5:
