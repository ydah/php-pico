--TEST--
associative key order vector 027
--FILE--
<?php
$items = ['left' => 37, 4 => 101, 'right' => 165];
unset($items[4]);
$items['tail'] = 166;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:37,165,166:1
