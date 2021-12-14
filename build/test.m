if isunix
    [status, result] = system('./ftdi_readWrite')
elseif ispc
    [status, result] = system('ftdi_readWrite.exe')
end