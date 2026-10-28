t1 = 2 + 3
t = t1
t2 = 10 * 4
x = t2
t3 = 20 / 2
t4 = t3 + 1
y = t4
read a
read b
t5 = a * b
t6 = t5 + 5
c = t6
t7 = a * b
t8 = t7 - 2
d = t8
t9 = a + b
t10 = a + b
t11 = t9 * t10
e = t11
L1:
if c > 0 goto L2
goto L3
L2:
if c == 5 goto L4
goto L5
L4:
goto L3
L5:
t12 = c - 1
c = t12
goto L1
L3:
write d
write e
write c
