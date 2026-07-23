--TEST--
array copy-on-write vector 024
--FILE--
<?php
$original = [25, 51, 77];
$copy = $original;
$copy[1] = 179; $copy[] = 180;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
25,51,77:25,179,77,180
