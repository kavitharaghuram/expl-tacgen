i = 2
L1:
if i > 0 goto L2
goto L3
L2:
j = 2
L4:
if j > 0 goto L5
goto L6
L5:
write j
t1 = j - 1
j = t1
goto L4
L6:
t2 = i - 1
i = t2
goto L1
L3:
