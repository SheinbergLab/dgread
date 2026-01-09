% buildmac.m - Build dg_read MEX file on macOS
%
% This script compiles dg_read.cpp and its dependencies into a MEX file
% for reading .dg, .dgz, and .lz4 data files.
%
% Requirements:
%   - MATLAB R2018a or later (for C++ MEX API)
%   - Xcode command line tools
%   - zlib (usually pre-installed on macOS)
%
% Usage:
%   >> cd /path/to/dg_source
%   >> buildmac
%
% The resulting MEX file can be used as:
%   >> data = dg_read('myfile.dg');
%   >> data = dg_read('myfile.dgz');
%   >> data = dg_read('myfile.lz4');

fprintf('Building dg_read MEX file for macOS...\n');

% Source files
sources = { ...
    'dg_read.cpp', ...
    'df.c', ...
    'dfutils.c', ...
    'dynio.c', ...
    'flip.c', ...
    'lz4.c', ...
    'lz4frame.c', ...
    'lz4hc.c', ...
    'lz4utils.c', ...
    'xxhash.c' ...
};

% Verify all source files exist
missing = {};
for i = 1:length(sources)
    if ~exist(sources{i}, 'file')
        missing{end+1} = sources{i}; %#ok<SAGROW>
    end
end

if ~isempty(missing)
    fprintf('ERROR: Missing source files:\n');
    for i = 1:length(missing)
        fprintf('  - %s\n', missing{i});
    end
    error('Please ensure all source files are in the current directory.');
end

% Verify required headers exist
headers = {'df.h', 'dynio.h', 'utilc.h', 'lz4.h', 'lz4frame.h', 'lz4hc.h', 'xxhash.h'};
missing_headers = {};
for i = 1:length(headers)
    if ~exist(headers{i}, 'file')
        missing_headers{end+1} = headers{i}; %#ok<SAGROW>
    end
end

if ~isempty(missing_headers)
    fprintf('WARNING: Missing header files:\n');
    for i = 1:length(missing_headers)
        fprintf('  - %s\n', missing_headers{i});
    end
    fprintf('Build may fail.\n\n');
end

% Compiler flags for C++ files
% -std=c++11    : Required for modern MEX API
% -Wno-deprecated: Suppress deprecation warnings  
cxxflags = 'CXXFLAGS=$CXXFLAGS -std=c++11 -Wno-deprecated';

% Compiler flags for C files
% These suppress common warnings in older C code:
% -Wno-implicit-function-declaration : Functions used before declared
% -Wno-incompatible-pointer-types    : Pointer type mismatches
% -Wno-int-conversion                : Int/pointer conversions
% -Wno-format                        : Printf format warnings
cflags = 'CFLAGS=$CFLAGS -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-format';

% Include current directory for headers
includes = '-I.';

% Link against zlib
libs = '-lz';

% Build the MEX file
try
    fprintf('Compiling...\n');
    mex('-v', cxxflags, cflags, includes, sources{:}, libs);
    fprintf('\n');
    fprintf('===========================================\n');
    fprintf('Build successful!\n');
    fprintf('===========================================\n');
    fprintf('\n');
    fprintf('Usage:\n');
    fprintf('  data = dg_read(''filename.dg'')   %% Plain DG file\n');
    fprintf('  data = dg_read(''filename.dgz'')  %% Gzip compressed\n');  
    fprintf('  data = dg_read(''filename.lz4'')  %% LZ4 compressed\n');
    fprintf('\n');
catch ME
    fprintf('\n');
    fprintf('===========================================\n');
    fprintf('Build FAILED\n');
    fprintf('===========================================\n');
    fprintf('Error: %s\n', ME.message);
    fprintf('\n');
    
    % Provide troubleshooting hints
    fprintf('Troubleshooting:\n');
    fprintf('  1. Ensure all source files are in the current directory:\n');
    fprintf('     %s\n', pwd);
    fprintf('  2. Check Xcode command line tools are installed:\n');
    fprintf('     xcode-select --install\n');
    fprintf('  3. For zlib issues on Apple Silicon, try:\n');
    fprintf('     brew install zlib\n');
    fprintf('  4. Check MATLAB version supports C++ MEX API (R2018a+)\n');
    fprintf('\n');
    
    % Re-throw for debugging
    rethrow(ME);
end
