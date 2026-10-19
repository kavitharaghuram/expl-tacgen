x = 3
L1:
if x > 0 goto L2
goto L3
L2:
t1 = x - 1
x = t1
if x == 1 goto L4
goto L5
L4:
goto L1
L5:
write x
goto L1
L3:
