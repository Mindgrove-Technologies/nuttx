/****************************************************************************
 * boards/risc-v/shakti/arty_a7/src/shakti_bringup.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdio.h>
#include <debug.h>
#include <errno.h>
#include <sys/stat.h>
#include <nuttx/board.h>
#include <nuttx/fs/fs.h>
#include <nuttx/input/buttons.h>
#include "mindgrove_spi.h"
#include <nuttx/spi/spi_transfer.h>
#include <nuttx/i2c/i2c_master.h>
#include "secure-iot.h"
#include <nuttx/spi/spi.h>
#include "mindgrove_gpio.h"
#include "mindgrove_i2c.h"



/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: shakti_bringup
 ****************************************************************************/

int mindgrove_bringup(void)
{

struct spi_dev_s *spi;
  int ret;

#if defined(CONFIG_MINDGROVE_SPI0)
  spi = mg_spibus_initialize(0);
  if (spi != NULL)
    {
#ifdef CONFIG_SPI_DRIVER
      ret = spi_register(spi, 0); /* Creates /dev/spi0 */
      if (ret < 0) _alert("ERROR: Failed to register SPI0: %d\n\r", ret);
#endif
    }
#endif

#if defined(CONFIG_MINDGROVE_SPI1)
  spi = mg_spibus_initialize(1);
  if (spi != NULL)
    {
#ifdef CONFIG_SPI_DRIVER
      ret = spi_register(spi, 1); /* Creates /dev/spi1 */
      if (ret < 0) _alert("ERROR: Failed to register SPI1: %d\n\r", ret);
#endif
    }
#endif

#if defined(CONFIG_MINDGROVE_SPI2)

  spi = mg_spibus_initialize(2);
  if (spi != NULL)
    {
#ifdef CONFIG_SPI_DRIVER
      ret = spi_register(spi, 2); /* Creates /dev/spi2 */
      if (ret < 0) _alert("ERROR: Failed to register SPI2: %d\n\r", ret);
#endif
    }
#endif

#if defined(CONFIG_MINDGROVE_SPI3)
  spi = mg_spibus_initialize(3);
  if (spi != NULL)
    {
#ifdef CONFIG_SPI_DRIVER
      ret = spi_register(spi, 3); /* Creates /dev/spi3 */
      if (ret < 0) _alert("ERROR: Failed to register SPI3: %d\n\r", ret);
#endif
    }
#endif

#ifdef CONFIG_DEV_GPIO

  ret = mindgrove_gpio_init();

  if (ret<0){
    serr("ERROR: Failed to initialize GPIO\n");
  }
  
#endif


#if defined(CONFIG_MINDGROVE_I2C0)

  FAR struct i2c_master_s *i2c0;

  i2c0 = mg_i2c_initialize(0);
  if (i2c0 != NULL)
    {
      ret = i2c_register(i2c0, 0);   /* Creates /dev/i2c0 */
      if (ret < 0)
        {
          _alert("ERROR: Failed to register I2C0: %d\n\r", ret);
        }
    }

#endif
#if defined(CONFIG_MINDGROVE_I2C1)

  FAR struct i2c_master_s *i2c1;

  i2c1 = mg_i2c_initialize(1);
  if (i2c1 != NULL)
    {
      ret = i2c_register(i2c1, 1);   /* Creates /dev/i2c0 */
      if (ret < 0)
        {
          _alert("ERROR: Failed to register I2C1: %d\n\r", ret);
        }
    }

#endif
  return 0;
}