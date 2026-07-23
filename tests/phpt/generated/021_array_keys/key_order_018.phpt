--TEST--
associative key order vector 018
--FILE--
<?php
$items = ['left' => 28, 4 => 74, 'right' => 120];
unset($items[4]);
$items['tail'] = 121;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:28,120,121:1
