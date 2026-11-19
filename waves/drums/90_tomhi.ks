/ High Tom
/ 500ms duration

N: ms 500
T: !N
E: e(T*(0-6.9%N))
/ pitch sweep 300->180 Hz
F: 180+120*e(T*(0-100%N))
D: hz F
P: +\D
/ clean fundamental only — parentheses prevent FM parsing
S: (s P)
/ tiny lowpassed thump at attack
Q: e(T*(0-220%N))
R: r T
C: 0.1 f R
W: w E*S+Q*C*.08

O: 1
W
