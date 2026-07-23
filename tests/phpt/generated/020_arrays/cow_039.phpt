--TEST--
array copy-on-write vector 039
--FILE--
<?php
$original = [40, 81, 122];
$copy = $original;
$copy[1] = 284; $copy[] = 285;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
40,81,122:40,284,122,285
