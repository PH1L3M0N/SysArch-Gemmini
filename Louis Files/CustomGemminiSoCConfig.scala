package chipyard
import org.chipsalliance.cde.config.Config

class CustomGemminiSoCConfig extends Config(
  new gemmini.GemminiCustomConfig ++
  new RocketConfig
)

class NoDebugCustomGemminiSoCConfig extends Config(
  new chipyard.config.WithNoDebug ++
  new CustomGemminiSoCConfig
)
