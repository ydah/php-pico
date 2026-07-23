--TEST--
array copy-on-write vector 035
--FILE--
<?php
$original = [36, 73, 110];
$copy = $original;
$copy[1] = 256; $copy[] = 257;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
36,73,110:36,256,110,257
