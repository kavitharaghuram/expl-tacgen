t1 = 2 + 3
t = t1
t2 = t * 4
x = t2
t3 = 10 / 2
t4 = t3 + 1
y = t4
read a
read b
t5 = b * 2
t6 = a + t5
c = t6
L1:
if c > 0 goto L2
goto L3
L2:
if c == 5 goto L4
goto L5
L4:
goto L3
L5:
t7 = c - 1
c = t7
goto L1
L3:
write c
