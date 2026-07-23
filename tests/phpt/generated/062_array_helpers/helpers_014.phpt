--TEST--
array helper vector 014
--FILE--
<?php
$values = [17, 35, 53];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
53,35,17:35,53:105:31535:53:17
