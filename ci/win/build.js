const process = require('process');
const cp = require('child_process');

class Config {
    constructor(name, cmd, args) {
        this.name = name;
        this.cmd = cmd;
        this.args = args;
    }
    
    execute() {
        let self = this;
        return new Promise(function(resolve, reject) {
            self.proc = cp.spawn(
                self.cmd, self.args, {
                    windowsVerbatimArguments: true,
                    windowsHide: true
                }
            );
            self.proc.stdout.on('data', (data) => {
                process.stdout.write(`[${self.name}:Out] ${data}`);
            });
            self.proc.stderr.on('data', (data) => {
                process.stderr.write(`[${self.name}:Err] ${data}`);
            });
            self.proc.on('exit', (code, signal) => {
                if (code != 0) {
                    reject(code);
                    return;
                }
                resolve(code);
                return;
            });
        });
    }
}

let configure = new Config("Config", "cmake", [
    '-H.',
    '-Bbuild',
    `-G"${process.env.CMAKE_GENERATOR}"`,
    `-T"${process.env.CMAKE_TOOLSET}"`,
    `-DWEBRTC_ROOT_DIR="${process.env.WEBRTC_PATH}"`,
    `-DDepsPath="${process.env.OBS_DEPENDENCIES_PATH}/${process.env.OBS_DEPENDENCIES_ARCH}"`,
    '-DCMAKE_INSTALL_PREFIX=".install"'
]);
configure.execute().then(function(result) {
    let appveyor_opts = [];
    if(process.env.APPVEYOR) {
        appveyor_opts = ['--', '/logger:"C:\\Program Files\\AppVeyor\\BuildAgent\\Appveyor.MSBuildLogger.dll"'];
    }
    
    let configs = [
        new Config("Debug", "cmake", ['--build', 'build', '--target', 'INSTALL', '--config', 'Debug'].concat(appveyor_opts)),
        new Config("MinSizeRel", "cmake", ['--build', 'build', '--target', 'INSTALL', '--config', 'MinSizeRel'].concat(appveyor_opts)),
        new Config("RelWithDebInfo", "cmake", ['--build', 'build', '--target', 'INSTALL', '--config', 'RelWithDebInfo'].concat(appveyor_opts)),
        new Config("Release", "cmake", ['--build', 'build', '--target', 'INSTALL', '--config', 'Release'].concat(appveyor_opts))
    ];
    let promises = [];
    for (config of configs) {
        promises.push(config.execute());
    }
    Promise.all(promises).then(function(result) {
        let packager = [
            new Config("7-Zip", "cmake", ['--build', 'build', '--target', 'PACKAGE_7Z', '--config', 'RelWithDebInfo'].concat(appveyor_opts)),
            new Config("Zip", "cmake", ['--build', 'build', '--target', 'PACKAGE_ZIP', '--config', 'Release'].concat(appveyor_opts))
        ];
        let promises = [];
        for (pack of packager) {
            promises.push(pack.execute());
        }
        Promise.all(promises).then(function(result) {
            process.exit(result);
        }).catch(function(result) {
            process.exit(result);
        });
    }).catch(function(result) {
        process.exit(result);
    });
}).catch(function(result) {
    process.exit(result);
});
