from flux.cli.plugin import CLIPlugin


class RediscoverGPUPlugin(CLIPlugin):
    """Add --multi-user option to configure a multi-user subinstance"""

    def __init__(self, prog):
        super().__init__(
            prog,
            "gpumode",
            "Option for setting AMD SMI compute partitioning. Choices are CPX, TPX, or SPX.",
        )

    def preinit(self, args, value):
        if self.prog in ("batch", "alloc"):
            gpumode = value.gpumode
            if gpumode == "TPX" or gpumode == "CPX":
                args.conf.update("resource.rediscover=true")
            elif gpumode == "SPX" or gpumode == None:
                pass
            else:
                raise ValueError("--gpumode can only be set to CPX, TPX, or SPX")

    def modify_jobspec(self, args, jobspec, value):
        if value.gpumode:
            jobspec.setattr("gpumode", value.gpumode)
    
