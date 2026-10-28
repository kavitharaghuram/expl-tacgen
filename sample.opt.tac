read a
read b
t5 = a * b
c = t5 + 5
d = t5 - 2
t9 = a + b
e = t9 * t9
t = 5
x = 40
y = 11
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
write d
write e
write c
