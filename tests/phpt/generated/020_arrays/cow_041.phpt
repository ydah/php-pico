--TEST--
array copy-on-write vector 041
--FILE--
<?php
$original = [42, 85, 128];
$copy = $original;
$copy[1] = 298; $copy[] = 299;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
42,85,128:42,298,128,299
