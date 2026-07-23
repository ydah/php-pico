--TEST--
object clone and property COW vector 009
--FILE--
<?php
class GeneratedClone009 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone009([12, 23]);
$copy = clone $original;
$copy->items[0] = 89;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
12,23:89,23
