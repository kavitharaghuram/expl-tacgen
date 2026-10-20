read a
read b
i = 0
t1 = a + b
x = t1
L1:
if i < 3 goto L2
goto L3
L2:
t2 = a + b
y = t2
write y
t3 = i + 1
i = t3
goto L1
L3:
write x
