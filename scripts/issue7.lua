-- Regression test for monome/iii#7: string building that hung stock iii at n=10.
-- Expected: both "ok" lines, then "not enough memory" (a Lua error, no reboot);
-- the REPL must still answer afterwards. The last line only runs when typed at the REPL.
for i=1,10 do local s="" for j=1,2^i do s=s.."!" end end print("n=10 ok", mem())
for i=1,13 do local s="" for j=1,2^i do s=s.."!" end end print("n=13 ok", mem())
s = string.rep("!", 300000)
print("still alive", mem())
