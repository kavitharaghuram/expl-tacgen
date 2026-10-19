i = 2
L1:
if i > 0 goto L2
goto L3
L2:
j = 3
L4:
if j > 0 goto L5
goto L6
L5:
j = j - 1
if j == 1 goto L7
goto L8
L7:
goto L4
L8:
if j == 0 goto L9
goto L10
L9:
goto L6
L10:
write j
goto L4
L6:
i = i - 1
goto L1
L3:
