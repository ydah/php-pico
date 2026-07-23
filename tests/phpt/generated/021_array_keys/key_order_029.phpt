--TEST--
associative key order vector 029
--FILE--
<?php
$items = ['left' => 39, 4 => 107, 'right' => 175];
unset($items[4]);
$items['tail'] = 176;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:39,175,176:1
