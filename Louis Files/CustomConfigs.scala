package gemmini

import org.chipsalliance.cde.config.{Config, Parameters}
import chisel3._
import freechips.rocketchip.diplomacy.LazyModule
import freechips.rocketchip.subsystem.SystemBusKey
import freechips.rocketchip.tile.BuildRoCC


object GemminiCustomConfigs {
  // Default configurations
  val defaultConfig = GemminiConfigs.defaultConfig
  val defaultFpConfig = GemminiFPConfigs.defaultFPConfig

  val sa64_mesh8_tile8_1024_256 = defaultConfig.copy(
    tileRows    = 8,
    tileColumns = 8,
    meshRows    = 64,
    meshColumns = 64,

    sp_capacity  = CapacityInKilobytes(1024),
    sp_banks    = 4,
    acc_banks    = 2,
    acc_capacity = CapacityInKilobytes(2048),
    has_training_convs = false,
    has_max_pool = false,
    has_nonlinear_activations = false,

    dma_maxbytes = 64,
    dma_buswidth = 128,
  )

  val sa64_mesh8_tile8_512_128 = defaultConfig.copy(
    tileRows    = 8,
    tileColumns = 8,
    meshRows    = 8,
    meshColumns = 8,

    sp_capacity  = CapacityInKilobytes(512),
    acc_capacity = CapacityInKilobytes(128),
  )

  val sa64_mesh8_tile8_128_64 = defaultConfig.copy(
    tileRows    = 8,
    tileColumns = 8,
    meshRows    = 8,
    meshColumns = 8,

    sp_capacity  = CapacityInKilobytes(128),
    acc_capacity = CapacityInKilobytes(64),
  )

  val sa64_mesh8_tile8_1024_128 = defaultConfig.copy(
    tileRows    = 8,
    tileColumns = 8,
    meshRows    = 8,
    meshColumns = 8,

    sp_capacity  = CapacityInKilobytes(1024),
    acc_capacity = CapacityInKilobytes(128),
  )

  val sa64_mesh8_tile8_256_64 = defaultConfig.copy(
    tileRows    = 8,
    tileColumns = 8,
    meshRows    = 8,
    meshColumns = 8,

    sp_capacity  = CapacityInKilobytes(256),
    acc_capacity = CapacityInKilobytes(64),
  )

  val sa64_mesh8_tile8_512_64 = defaultConfig.copy(
    tileRows    = 8,
    tileColumns = 8,
    meshRows    = 8,
    meshColumns = 8,

    sp_capacity  = CapacityInKilobytes(512),
    acc_capacity = CapacityInKilobytes(64),
  )

  // Create your own configs here
  val baselineInferenceConfig = defaultConfig.copy(
    has_training_convs = false,
  )

  val highPerfInferenceConfig = defaultConfig.copy(
    meshRows = 32,
    meshColumns = 32,

    has_training_convs = false,

    sp_capacity = CapacityInKilobytes(512),
    acc_capacity = CapacityInKilobytes(128),
  )

  val trainingConfig = defaultFpConfig.copy(
    inputType = Float(expWidth = 8, sigWidth = 24),
    accType = Float(expWidth = 8, sigWidth = 24),

    meshRows = 8,
    meshColumns = 8,

    has_training_convs = true,
    has_max_pool =  false,

    sp_capacity = CapacityInKilobytes(512),
    acc_capacity = CapacityInKilobytes(128),
  )

  val ibertInferenceConfig = defaultConfig.copy(
    has_training_convs = false,
    has_max_pool =  false,
    has_normalizations = true,

    acc_capacity = CapacityInKilobytes(128),
  )

  val unifiedMemConfig = defaultConfig.copy(
    has_training_convs = false,
    has_max_pool = false,
    use_tl_ext_mem = true,
    sp_singleported = false,
    spad_read_delay = 8,
    use_shared_ext_mem = true,
    acc_sub_banks = 1
  )

  // Specify which of your custom configs you want to build here
  val customConfig = sa64_mesh8_tile8_1024_256
}

class GemminiCustomConfig[T <: Data : Arithmetic, U <: Data, V <: Data](
  gemminiConfig: GemminiArrayConfig[T,U,V] = GemminiCustomConfigs.customConfig
) extends Config((site, here, up) => {
  case BuildRoCC => up(BuildRoCC) ++ Seq(
    (p: Parameters) => {
      implicit val q = p
      val gemmini = LazyModule(new Gemmini(gemminiConfig))
      gemmini
    }
  )
})


