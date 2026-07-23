--TEST--
associative key order vector 008
--FILE--
<?php
$items = ['left' => 18, 4 => 44, 'right' => 70];
unset($items[4]);
$items['tail'] = 71;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:18,70,71:1
