--TEST--
associative key order vector 024
--FILE--
<?php
$items = ['left' => 34, 4 => 92, 'right' => 150];
unset($items[4]);
$items['tail'] = 151;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:34,150,151:1
