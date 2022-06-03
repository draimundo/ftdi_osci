[ch1, ch2, ch3] = importfile("out.csv", [1, Inf]);
hold on;

var=ch1;
f = 250E3;
T = 1/f;
t = 1:length(var);
t = t*T;
res = 0xFFF;
pos = 0x7FF;
neg = 0x800;

var = ch1;
vals = zeros(1,length(ch1));
for i =1:length(var)
    vals(i) = double(bitand(var(i),pos))-double(bitand(var(i),neg));
end
vals = (((vals+2048)/4095)*5)-2.5;
vals = vals;
plot(t,vals,'DisplayName','CH1[V]')
xlabel('t [s]')
ylabel('[V]')
ylim([-2.5,2.5])

var = ch2;
vals = zeros(1,length(ch2));
for i =1:length(var)
    vals(i) = double(bitand(var(i),pos))-double(bitand(var(i),neg));
end
vals = (((vals+2048)/4095)*5)-2.5;
vals = vals;
plot(t,vals,'DisplayName','CH2[V]')
xlabel('t [s]')

yyaxis right
var = ch3;
vals = zeros(1,length(ch3));
for i =1:length(var)
    vals(i) = double(bitand(var(i),pos))-double(bitand(var(i),neg));
end
vals = (((vals+2048)/4095)*5)-2.5;
vals = vals/0.5/20*1e3;
plot(t,vals,'DisplayName','CH3[mA]')
xlabel('t [s]')
ylabel('[mA]')
ylim([-50,50])
legend