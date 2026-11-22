/ 808 Kick — 141->50Hz sweep, 300ms
N: ms 300
T: !N
E: e(T*(0-6.9%N))
/ pitch sweep 141->50Hz
F: 50+91*e(T*(0-60%N))
D: hz F
P: +\D
S: s P
/ short attack noise thump
Q: e(T*(0-300%N))
R: r T
C: 0.04 f R
W: w (E*S)+(Q*C*.1)

O: 1
META: \"mode:sample one_shot:1\"
W
