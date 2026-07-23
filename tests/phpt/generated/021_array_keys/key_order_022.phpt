--TEST--
associative key order vector 022
--FILE--
<?php
$items = ['left' => 32, 4 => 86, 'right' => 140];
unset($items[4]);
$items['tail'] = 141;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:32,140,141:1
