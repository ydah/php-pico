--TEST--
array copy-on-write vector 031
--FILE--
<?php
$original = [32, 65, 98];
$copy = $original;
$copy[1] = 228; $copy[] = 229;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
32,65,98:32,228,98,229
