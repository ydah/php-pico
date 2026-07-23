--TEST--
associative key order vector 010
--FILE--
<?php
$items = ['left' => 20, 4 => 50, 'right' => 80];
unset($items[4]);
$items['tail'] = 81;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:20,80,81:1
