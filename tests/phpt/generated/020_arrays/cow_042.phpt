--TEST--
array copy-on-write vector 042
--FILE--
<?php
$original = [43, 87, 131];
$copy = $original;
$copy[1] = 305; $copy[] = 306;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
43,87,131:43,305,131,306
