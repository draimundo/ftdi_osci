%%
fs = 250e3;
f1 = 1e3;
f2 = 20e3;
nSamples = 20e3;
amplitude = 0.2;
offset = 0.5;

out = zeros(nSamples,1);
for t = 1:nSamples
    out(t) = (amplitude*sin(2*pi*t*f1/fs) + amplitude*sin(2*pi*t*f2/fs) + offset)*0xFFF;
end

plot((1:nSamples)./fs, out)

%%
figure;
Out = fft(out);
P2 = abs(Out/nSamples);
P2 = (((P2+2048)/4095)*5)-2.5;
P1 = P2(1:nSamples/2+1);
P1(2:end-1) = 2*P1(2:end-1);

f = fs*(0:(nSamples/2))/nSamples;
plot(f,P1) 

%%
hold on;
var = ch1(1000:1:end);
var = (((var+2048)/4095)*5)-2.5;
Out = fft(var.*hanning(length(var)));
P2 = abs(Out/length(Out));
P1 = P2(1:length(Out)/2+1);
P1(2:end-1) = 2*P1(2:end-1);

f = fs*(0:(length(Out)/2))/nSamples;
plot(f,P1)

%%
hold on;
var = ch3(1000:1:end);
var = (((var+2048)/4095)*5)-2.5;
Out = fft(var.*hanning(length(var)));
P2 = abs(Out/length(Out));
P1 = P2(1:length(Out)/2+1);
P1(2:end-1) = 2*P1(2:end-1);

f = fs*(0:(length(Out)/2))/nSamples;
plot(f,P1)