--TEST--
array helper vector 019
--FILE--
<?php
$values = [22, 45, 68];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
68,45,22:45,68:135:67320:68:22
