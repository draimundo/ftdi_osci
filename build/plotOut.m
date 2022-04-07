[ch1, ch2, ch3] = importfile("out.csv", [1, Inf]);
hold on;
var = ch3;
f = 250E3;
T = 1/f;
t = 1:length(var);
t = t*T;
res = 0xFFF;
pos = 0x7FF;
neg = 0x800;

vals = zeros(1,length(var));
for i =1:length(var)
    vals(i) = double(bitand(var(i),pos))-double(bitand(var(i),neg));
end
vals = (((vals+2048)/4095)*5)-2.5;
vals = vals/2;
plot(t,vals)
xlabel('t [s]')
ylabel('ch_1 [V]')