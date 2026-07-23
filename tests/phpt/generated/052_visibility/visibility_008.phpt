--TEST--
private and protected member access vector 008
--FILE--
<?php
class GeneratedVisibility008 {
    private $left = 60; protected $right = 46;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility008())->total();
--EXPECT--
106
