read x
read y
if x > 0 goto L3
goto L2
L3:
if y > 0 goto L1
goto L2
L1:
write 1
L2:
if x > 0 goto L4
goto L6
L6:
if y > 0 goto L4
goto L5
L4:
write 2
L5:
if x == 0 goto L8
goto L7
L7:
write 3
L8:
if x > 0 goto L12
goto L11
L12:
if y > 0 goto L9
goto L11
L11:
if x == 5 goto L9
goto L10
L9:
write 4
L10:
