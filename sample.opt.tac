read a
read b
t5 = b * 2
c = a + t5
t = 5
x = 20
y = 6
L1:
if c > 0 goto L2
goto L3
L2:
if c == 5 goto L4
goto L5
L4:
goto L3
L5:
c = c - 1
goto L1
L3:
write c
