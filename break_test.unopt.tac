x = 5
L1:
if x > 0 goto L2
goto L3
L2:
if x == 3 goto L4
goto L5
L4:
goto L3
L5:
write x
t1 = x - 1
x = t1
goto L1
L3:
write 0
