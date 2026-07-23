--TEST--
nested array copy-on-write vector 017
--FILE--
<?php
$tree = ['node' => ['value' => 69, 'items' => [70]]];
$copy = $tree;
$copy['node']['value'] += 5;
$copy['node']['items'][] = 76;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
69:74:70:70,76
