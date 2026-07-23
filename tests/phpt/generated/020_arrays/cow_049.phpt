--TEST--
array copy-on-write vector 049
--FILE--
<?php
$original = [50, 101, 152];
$copy = $original;
$copy[1] = 354; $copy[] = 355;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
50,101,152:50,354,152,355
