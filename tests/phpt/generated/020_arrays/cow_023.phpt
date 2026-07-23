--TEST--
array copy-on-write vector 023
--FILE--
<?php
$original = [24, 49, 74];
$copy = $original;
$copy[1] = 172; $copy[] = 173;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
24,49,74:24,172,74,173
